const { Client } = require("@notionhq/client")

const notion = new Client({
  auth: process.env.NOTION_TOKEN
})

const issue = JSON.parse(process.env.ISSUE_JSON)

async function main() {
  await notion.pages.create({
    parent: {
      database_id: process.env.NOTION_DATABASE_ID
    },

    properties: {
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
  })
}

main().catch(err => {
  console.error("Notion sync failed:")
  console.error(JSON.stringify(err, null, 2))
  process.exit(1)
})
