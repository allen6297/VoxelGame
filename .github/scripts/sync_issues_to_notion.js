console.log("Running Notion issue sync")
console.log(`Issue #${issue.number}: ${issue.title}`)
console.log(`Labels: ${issue.labels.map(label => label.name).join(", ")}`)

const { Client } = require("@notionhq/client")

const notion = new Client({
  auth: process.env.NOTION_TOKEN
})

const issue = JSON.parse(process.env.ISSUE_JSON)

const properties = {
  Name: {
    title: [{ text: { content: issue.title } }]
  },

  Description: {
    rich_text: [
      {
        text: {
          content: issue.body || "No description."
        }
      }
    ]
  },

  Tags: {
    multi_select: issue.labels.map(label => ({
      name: label.name
    }))
  },

  "Issue Number": {
    number: issue.number
  },

  Status: {
    select: {
      name: issue.state === "closed" ? "Done" : "Open"
    }
  },

  "Github Issue": {
    url: issue.html_url
  }
}

async function findExistingPage() {
  const response = await notion.databases.query({
    database_id: process.env.NOTION_DATABASE_ID,
    filter: {
      property: "Issue Number",
      number: {
        equals: issue.number
      }
    }
  })

  return response.results[0]
}

async function main() {
  const existingPage = await findExistingPage()

  if (existingPage) {
    await notion.pages.update({
      page_id: existingPage.id,
      properties
    })

    console.log(`Updated Notion issue #${issue.number}`)
  } else {
    await notion.pages.create({
      parent: {
        database_id: process.env.NOTION_DATABASE_ID
      },
      properties
    })

    console.log(`Created Notion issue #${issue.number}`)
  }
}

main().catch(err => {
  console.error("Notion sync failed:")
  console.error(JSON.stringify(err, null, 2))
  process.exit(1)
})
